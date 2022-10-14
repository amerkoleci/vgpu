// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgpu_driver.h"

#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#elif defined(_WIN32)
#   include <Windows.h>
#endif

#define MAX_MESSAGE_SIZE 1024

static void VGPU_DefaultLogCallback(vgpu_log_level level, const char* message)
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

void vgpuLogWarn(const char* format, ...)
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

void vgpu_set_log_callback(vgpu_log_callback func)
{
    s_LogFunc = func;
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

vgpu_bool vgpuIsSupported(VGPUBackendType backend)
{
    for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
    {
        if (!drivers[i])
            break;

        if (drivers[i]->backend == backend)
        {
            return drivers[i]->isSupported();
        }
    }

    return false;
}

VGPUDevice vgpuCreateDevice(const VGPUDeviceDesc* desc)
{
    NULL_RETURN_NULL(desc);

    VGPUBackendType backend = desc->preferredBackend;

retry:
    if (backend == VGPUBackendType_Default)
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->isSupported())
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
                if (drivers[i]->isSupported())
                {
                    return drivers[i]->createDevice(desc);
                }
                else
                {
                    vgpuLogWarn("Wanted API not supported, fallback to default");
                    backend = VGPUBackendType_Default;
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

vgpu_bool vgpuQueryFeature(VGPUDevice device, VGPUFeature feature, void* pInfo, uint32_t infoSize)
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
    def.width = _VGPU_DEF(def.width, 1);
    def.height = _VGPU_DEF(def.height, 1);
    def.depthOrArraySize = _VGPU_DEF(def.depthOrArraySize, 1);
    def.mipLevels = _VGPU_DEF(def.mipLevels, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* pInitialData)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);

    VGPU_ASSERT(desc_def.width > 0 && desc_def.height > 0 && desc_def.mipLevels >= 0);

    const bool is3D = desc_def.type == VGPUTexture_Type3D;
    const bool isDepthStencil = vgpuIsDepthStencilFormat(desc_def.format);
    bool isCube = false;

    if (desc_def.type == VGPUTexture_Type2D &&
        desc_def.width == desc_def.height &&
        desc_def.depthOrArraySize >= 6)
    {
        isCube = true;
    }

    uint32_t mipLevels = desc_def.mipLevels;
    if (mipLevels == 0)
    {
        if (is3D)
        {
            mipLevels = vgpuNumMipLevels(desc_def.width, desc_def.height, desc_def.depthOrArraySize);
        }
        else
        {
            mipLevels = vgpuNumMipLevels(desc_def.width, desc_def.height, 1);
        }
    }

    if (desc_def.sampleCount > 1u)
    {
        if (isCube)
        {
            vgpuLogWarn("Cubemap texture cannot be multisample");
            return NULL;
        }

        if (is3D)
        {
            vgpuLogWarn("3D texture cannot be multisample");
            return NULL;
        }

        if (mipLevels > 1)
        {
            vgpuLogWarn("Multisample texture cannot have mipmaps");
            return NULL;
        }
    }

    if (isDepthStencil && mipLevels > 1)
    {
        vgpuLogWarn("Depth texture cannot have mipmaps");
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
        vgpuLogWarn("Cannot create Depth texture with ShaderWrite usage");
        return NULL;
    }

    return device->createTexture(device->driverData, &desc_def, pInitialData);
}

VGPUTexture vgpuCreateTexture2D(VGPUDevice device, uint32_t width, uint32_t height, VGPUTextureFormat format, uint32_t mipLevels, VGPUTextureUsage usage, const void* pInitialData)
{
    VGPUTextureDesc desc = {
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = usage,
        .width = width,
        .height = height,
        .depthOrArraySize = 1u,
        .mipLevels = mipLevels,
        .sampleCount = 1u
    };

    return vgpuCreateTexture(device, &desc, pInitialData);
}

VGPUTexture vgpuCreateTextureCube(VGPUDevice device, uint32_t size, VGPUTextureFormat format, uint32_t mipLevels, const void* pInitialData)
{
    VGPUTextureDesc desc = {
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = VGPUTextureUsage_ShaderRead,
        .width = size,
        .height = size,
        .depthOrArraySize = 6u,
        .mipLevels = mipLevels,
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

    VGPURenderPipelineDesc desc_def = _vgpuRenderPipelineDescDef(desc);
    return device->createRenderPipeline(device->driverData, &desc_def);
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

void vgpuDispatch1D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t groupSizeX)
{
    vgpuDispatch(commandBuffer, _VGPU_DivideByMultiple(threadCountX, groupSizeX), 1, 1);
}

void vgpuDispatch2D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY)
{
    vgpuDispatch(commandBuffer,
        _VGPU_DivideByMultiple(threadCountX, groupSizeX),
        _VGPU_DivideByMultiple(threadCountY, groupSizeY),
        1
    );
}

void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    commandBuffer->dispatch(commandBuffer->driverData, groupCountX, groupCountY, groupCountZ);
}

void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint32_t indirectBufferOffset)
{
    NULL_RETURN(indirectBuffer);

    commandBuffer->dispatchIndirect(commandBuffer->driverData, indirectBuffer, indirectBufferOffset);
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

    commandBuffer->setViewports(commandBuffer->driverData, viewports, count);
}

void vgpuSetScissorRects(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPURect* scissorRects)
{
    VGPU_ASSERT(scissorRects);
    VGPU_ASSERT(count < VGPU_MAX_VIEWPORTS_AND_SCISSORS);

    commandBuffer->setScissorRects(commandBuffer->driverData, scissorRects, count);
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
    { VGPUTextureFormat_Undefined,      "Undefined",           0,   0,  VGPU_TEXTURE_FORMAT_KIND_INTEGER,      false, false, false, false, false, false, false, false },
    { VGPUTextureFormat_R8UNorm,        "R8_UNORM",          1,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R8SNorm,        "R8_SNORM",          1,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R8UInt,         "R8_UINT",           1,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R8SInt,         "R8_SINT",           1,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },

    { VGPUTextureFormat_R16UInt,          "R16_UINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R16SInt,          "R16_SINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },
    { VGPUTextureFormat_R16UNorm,         "R16_UNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R16SNorm,         "R16_SNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R16Float,         "R16_FLOAT",         2,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  false, false, false, false, false, true,  false },
    { VGPUTextureFormat_RG8UInt,          "RG8_UINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG8SInt,          "RG8_SINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, true,  false },
    { VGPUTextureFormat_RG8UNorm,         "RG8_UNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG8SNorm,         "RG8_SNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },

    { VGPUTextureFormat_BGRA4UNorm,       "BGRA4_UNORM",       2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_B5G6R5UNorm,      "B5G6R5_UNORM",      2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  false, false, false, false, false },
    { VGPUTextureFormat_B5G5R5A1UNorm,    "B5G5R5A1_UNORM",    2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },

    { VGPUTextureFormat_R32UInt,          "R32_UINT",          4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_R32SInt,          "R32_SINT",          4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },
    { VGPUTextureFormat_R32Float,         "R32_FLOAT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  false, false, false, false, false, true,  false },
    { VGPUTextureFormat_RG16UInt,         "RG16_UINT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG16SInt,         "RG16_SINT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, true,  false },
    { VGPUTextureFormat_RG16UNorm,        "RG16_UNORM",        4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG16SNorm,        "RG16_SNORM",        4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG16Float,        "RG16_FLOAT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  true,  false, false, false, false, true,  false },

    { VGPUTextureFormat_RGBA8UInt,        "RGBA8_UINT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA8SInt,        "RGBA8_SINT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  true,  true,  false, false, true,  false },
    { VGPUTextureFormat_RGBA8UNorm,       "RGBA8_UNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA8UNormSrgb,      "SRGBA8_UNORM",      4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
    { VGPUTextureFormat_RGBA8SNorm,       "RGBA8_SNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BGRA8UNorm,       "BGRA8_UNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BGRA8UNormSrgb,      "SBGRA8_UNORM",      4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },

    { VGPUTextureFormat_RGB10A2UNorm,   "R10G10B10A2_UNORM",    4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,          true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RG11B10Float,   "R11G11B10_FLOAT",      4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,     true,  true,  true,  false, false, false, false, false },
    { VGPUTextureFormat_RGB9E5Float,    "RGB9E5Float",          4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,     true,  true,  true,  false, false, false, false, false },

    { VGPUTextureFormat_RG32UInt,         "RG32_UINT",         8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_RG32SInt,         "RG32_SINT",         8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  false, false, false, false, true,  false },
    { VGPUTextureFormat_RG32Float,        "RG32_FLOAT",        8,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,      true,  true,  false, false, false, false, true,  false },
    { VGPUTextureFormat_RGBA16UInt,       "RGBA16_UINT",       8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA16SInt,       "RGBA16_SINT",       8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, true,  false },
    { VGPUTextureFormat_RGBA16UNorm,      "RGBA16_UNORM",      8,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,           true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA16SNorm,      "RGBA16_SNORM",      8,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,           true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA16Float,      "RGBA16_FLOAT",      8,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,      true,  true,  true,  true,  false, false, true,  false },

    { VGPUTextureFormat_RGBA32UInt,       "RGBA32_UINT",       16,  1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_RGBA32SInt,       "RGBA32_SINT",       16,  1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, true,  false },
    { VGPUTextureFormat_RGBA32Float,      "RGBA32_FLOAT",      16,  1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,      true,  true,  true,  true,  false, false, true,  false },

    { VGPUTextureFormat_Depth16UNorm,           "Depth16UNorm", 2,   1, VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL, false, false, false, false, true,  false, false, false },
    { VGPUTextureFormat_Depth24UNormStencil8,   "D24S8",         4,   1, VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL, false, false, false, false, true,  true,  false, false },
    { VGPUTextureFormat_Depth32Float,           "D32",           4,   1, VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL, false, false, false, false, true,  false, false, false },
    { VGPUTextureFormat_Depth32FloatStencil8,   "D32S8",         8,   1, VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL, false, false, false, false, true,  true,  false, false },

    { VGPUTextureFormat_BC1UNorm,       "BC1UNorm",         8,   4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BC1UNormSrgb,   "BC1UNormSrgb",    8,   4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
    { VGPUTextureFormat_BC2UNorm,       "BC2UNorm",         16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BC2UNormSrgb,   "BC2UNormSrgb",    16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
    { VGPUTextureFormat_BC3UNorm,       "BC3UNorm",         16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BC3UNormSrgb,   "BC3UNormSrgb",    16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
    { VGPUTextureFormat_BC4UNorm,       "BC4UNorm",         8,   4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_BC4SNorm,       "BC4SNorm",         8,   4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGPUTextureFormat_BC5UNorm,       "BC5UNorm",         16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_BC5SNorm,       "BC5SNorm",         16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGPUTextureFormat_BC6HUFloat,     "BC6HUFloat",       16,  4, VGPU_TEXTURE_FORMAT_KIND_FLOAT, true,  true,  true,  false, false, false, false, false },
    { VGPUTextureFormat_BC6HSFloat,     "BC6HSFloat",       16,  4, VGPU_TEXTURE_FORMAT_KIND_FLOAT, true,  true,  true,  false, false, false, true,  false },
    { VGPUTextureFormat_BC7UNorm,       "BC7UNorm",         16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGPUTextureFormat_BC7UNormSrgb,   "BC7UNormSrgb",    16,  4, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
};

void vgpuGetFormatInfo(VGPUTextureFormat format, const VGPUFormatInfo* pInfo)
{
    VGPU_ASSERT(pInfo);

    //static_assert(sizeof(c_FormatInfo) / sizeof(VGFXFormatInfo) == _VGFXTextureFormat_Count,
    //    "The format info table doesn't have the right number of elements");

    if (format >= _VGPUTextureFormat_Count)
    {
        // UNKNOWN
        pInfo = &c_FormatInfo[0];
        return;
    }

    pInfo = &c_FormatInfo[(uint32_t)format];
    VGPU_ASSERT(pInfo->format == format);
}

vgpu_bool vgpuIsDepthFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth;
}

vgpu_bool vgpuIsStencilFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasStencil;
}

vgpu_bool vgpuIsDepthStencilFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth || c_FormatInfo[(uint32_t)format].hasStencil;
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
