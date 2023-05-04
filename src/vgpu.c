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

static VGPULogCallback s_LogFunc = NULL;
static void* s_userData = NULL;

void vgpu_log_info(const char* format, ...) {
    if (!s_LogFunc)
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_INFO, msg, s_userData);
}

void vgpu_log_warn(const char* format, ...) {
    if (!s_LogFunc)
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_WARN, msg, s_userData);
}

void vgpu_log_error(const char* format, ...) {
    if (!s_LogFunc)
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_ERROR, msg, s_userData);
}

void vgpuSetLogCallback(VGPULogCallback func, void* userData) {
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

const VGPUAllocationCallbacks VGPU_DEFAULT_ALLOC_CB = { vgpu_default_alloc, vgpu_default_free, NULL };

const VGPUAllocationCallbacks* VGPU_ALLOC_CB = &VGPU_DEFAULT_ALLOC_CB;

void vgpuSetAllocationCallbacks(const VGPUAllocationCallbacks* callback)
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
#if defined(VGPU_WEBGPU_DRIVER)
    &WebGPU_driver,
#endif
    NULL
};

#define NULL_RETURN(name) if (name == NULL) { return; }
#define NULL_RETURN_NULL(name) if (name == NULL) { return NULL; }

VGPUBool32 vgpuIsBackendSupported(VGPUBackend backend)
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

VGPUDevice vgpuCreateDevice(const VGPUDeviceDescriptor* desc)
{
    if (!desc) {
        vgpu_log_warn("vgpu_init: Invalid config");
        return false;
    }

    VGPUDevice device = NULL;
    VGPUBackend backend = desc->preferredBackend;

retry:
    if (backend == _VGPU_BACKEND_DEFAULT)
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->is_supported())
            {
                device = drivers[i]->createDevice(desc);
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
                    device = drivers[i]->createDevice(desc);
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

    return device;
}

void vgpuDestroyDevice(VGPUDevice device)
{
    NULL_RETURN(device);

    device->destroyDevice(device);
}

uint64_t vgpuFrame(VGPUDevice device)
{
    VGPU_ASSERT(device);

    return device->frame(device->driverData);
}

void vgpuWaitIdle(VGPUDevice device)
{
    device->waitIdle(device->driverData);
}

VGPUBackend vgpuGetBackend(VGPUDevice device)
{
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
    VGPU_ASSERT(device);
    NULL_RETURN(properties);

    device->getAdapterProperties(device->driverData, properties);
}

void vgpuGetLimits(VGPUDevice device, VGPULimits* limits)
{
    VGPU_ASSERT(device);
    NULL_RETURN(limits);

    device->getLimits(device->driverData, limits);
}

void vgpuSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(commandBuffers);
    VGPU_ASSERT(count);

    device->submit(device->driverData, commandBuffers, count);
}

/* Buffer */
static vgpu_buffer_desc _vgpu_buffer_desc_def(const vgpu_buffer_desc* desc)
{
    vgpu_buffer_desc def = *desc;
    def.size = _VGPU_DEF(def.size, 4);
    return def;
}

vgpu_buffer* vgpuCreateBuffer(VGPUDevice device, const vgpu_buffer_desc* desc, const void* init_data)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    vgpu_buffer_desc desc_def = _vgpu_buffer_desc_def(desc);
    return device->createBuffer(device->driverData, &desc_def, init_data);
}

void vgpuDestroyBuffer(VGPUDevice device, vgpu_buffer* buffer)
{
    VGPU_ASSERT(device);
    NULL_RETURN(buffer);

    device->destroyBuffer(device->driverData, buffer);
}

VGPUDeviceAddress vgpuGetBufferDeviceAddress(VGPUDevice device, vgpu_buffer* buffer)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(buffer);

    return device->buffer_get_device_address(buffer);
}

void vgpuSetBufferLabel(VGPUDevice device, vgpu_buffer* buffer, const char* label)
{
    VGPU_ASSERT(device);
    NULL_RETURN(buffer);

    device->buffer_set_label(device->driverData, buffer, label);
}

/* Texture */
static VGPUTextureDesc _vgpuTextureDescDef(const VGPUTextureDesc* desc)
{
    VGPUTextureDesc def = *desc;
    def.type = _VGPU_DEF(def.type, VGPUTexture_Type2D);
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_RGBA8Unorm);
    //def.usage = _VGPU_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.width = _VGPU_DEF(def.width, 1);
    def.height = _VGPU_DEF(def.height, 1);
    def.depthOrArrayLayers = _VGPU_DEF(def.depthOrArrayLayers, 1);
    def.mipLevelCount = _VGPU_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* init_data)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);

    VGPU_ASSERT(desc_def.width > 0 && desc_def.height > 0 && desc_def.mipLevelCount >= 0);

    const bool is3D = desc_def.type == VGPUTexture_Type3D;
    const bool isDepthStencil = vgpuIsDepthStencilFormat(desc_def.format);
    bool isCube = false;

    if (desc_def.type == VGPUTexture_Type2D &&
        desc_def.width == desc_def.height &&
        desc_def.depthOrArrayLayers >= 6)
    {
        isCube = true;
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

        if (desc_def.mipLevelCount > 1)
        {
            vgpu_log_warn("Multisample texture cannot have mipmaps");
            return NULL;
        }
    }

    if (isDepthStencil && desc_def.mipLevelCount > 1)
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

    return device->createTexture(device->driverData, &desc_def, init_data);
}

void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture)
{
    VGPU_ASSERT(device);
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
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUSamplerDesc desc_def = _vgpuSamplerDescDef(desc);
    return device->createSampler(device->driverData, &desc_def);
}

void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler)
{
    VGPU_ASSERT(device);
    NULL_RETURN(sampler);

    device->destroySampler(device->driverData, sampler);
}

/* Shader Module */
VGPUShaderModule vgpuCreateShaderModule(VGPUDevice device, const void* pCode, size_t codeSize)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(pCode);

    return device->createShaderModule(device->driverData, pCode, codeSize);
}

void vgpuDestroyShaderModule(VGPUDevice device, VGPUShaderModule module)
{
    VGPU_ASSERT(device);
    NULL_RETURN(module);

    device->destroyShaderModule(device->driverData, module);
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

VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->vertex);

    VGPURenderPipelineDesc desc_def = _vgpuRenderPipelineDescDef(desc);
    return device->createRenderPipeline(device->driverData, &desc_def);
}

VGPUPipeline vgpuCreateComputePipeline(VGPUDevice device, const VGPUComputePipelineDescriptor* descriptor)
{
    VGPU_ASSERT(device);

    NULL_RETURN_NULL(descriptor);
    VGPU_ASSERT(descriptor->shader);

    return device->createComputePipeline(device->driverData, descriptor);
}

VGPUPipeline vgpuCreateRayTracingPipeline(VGPUDevice device, const VGPURayTracingPipelineDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    return device->createRayTracingPipeline(device->driverData, desc);
}

void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline)
{
    VGPU_ASSERT(device);
    NULL_RETURN(pipeline);

    device->destroyPipeline(device->driverData, pipeline);
}

/* SwapChain */
static VGPUSwapChainDesc _vgpuSwapChainDescDef(const VGPUSwapChainDesc* desc)
{
    VGPUSwapChainDesc def = *desc;
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_BGRA8Unorm);
    def.presentMode = _VGPU_DEF(def.presentMode, VGPUPresentMode_Immediate);
    return def;
}

VGPUSwapChain* vgpuCreateSwapChain(VGPUDevice device, void* window, const VGPUSwapChainDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(window);
    NULL_RETURN_NULL(desc);

    VGPUSwapChainDesc def = _vgpuSwapChainDescDef(desc);
    return device->createSwapChain(device->driverData, window, &def);
}

void vgpuDestroySwapChain(VGPUDevice device, VGPUSwapChain* swapChain)
{
    VGPU_ASSERT(device);
    NULL_RETURN(swapChain);

    device->destroySwapChain(device->driverData, swapChain);
}

VGPUPixelFormat vgpuGetSwapChainFormat(VGPUDevice device, VGPUSwapChain* swapChain)
{
    VGPU_ASSERT(device);

    return device->getSwapChainFormat(device->driverData, swapChain);
}

/* Commands */
VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, VGPUCommandQueue queueType, const char* label)
{
    VGPU_ASSERT(device);

    return device->beginCommandBuffer(device->driverData, queueType, label);
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

void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports)
{
    VGPU_ASSERT(count > 0);
    VGPU_ASSERT(viewports);

    commandBuffer->setViewports(commandBuffer->driverData, count, viewports);
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

void vgpuSetStencilReference(VGPUCommandBuffer commandBuffer, uint32_t reference)
{
    commandBuffer->setStencilReference(commandBuffer->driverData, reference);
}

void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance)
{
    commandBuffer->draw(commandBuffer->driverData, vertexStart, vertexCount, instanceCount, firstInstance);
}

void vgpuDrawIndexed(VGPUCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    commandBuffer->drawIndexed(commandBuffer->driverData, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

// Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
static const VGPUPixelFormatInfo c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
    { VGPUTextureFormat_Undefined,      "Undefined",            0,   0, 0,  _VGPUTextureFormatKind_Force32 },
    { VGPUTextureFormat_R8Unorm,        "R8Unorm",              1,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_R8Snorm,        "R8Snorm",              1,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_R8Uint,         "R8Uint",               1,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_R8Sint,         "R8USnt",               1,   1, 1, VGPUPixelFormatKind_Sint },

    { VGPUTextureFormat_R16Unorm,         "R16Unorm",           2,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_R16Snorm,         "R16Snorm",           2,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_R16Uint,          "R16Uint",            2,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_R16Sint,          "R16Sint",            2,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_R16Float,         "R16Float",           2,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_RG8Unorm,         "RG8Unorm",           2,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_RG8Snorm,         "RG8Snorm",           2,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_RG8Uint,          "RG8Uint",            2,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RG8Sint,          "RG8Sint",            2,   1, 1, VGPUPixelFormatKind_Sint },

    { VGPUTextureFormat_BGRA4Unorm,       "BGRA4Unorm",         2,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_B5G6R5Unorm,      "B5G6R5Unorm",        2,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_B5G5R5A1Unorm,    "B5G5R5A1Unorm",      2,   1, 1, VGPUPixelFormatKind_Unorm },

    { VGPUTextureFormat_R32Uint,          "R32Uint",            4,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_R32Sint,          "R32Sint",            4,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_R32Float,         "R32Float",           4,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_RG16Unorm,        "RG16Unorm",          4,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_RG16Snorm,        "RG16Snorm",          4,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_RG16Uint,         "RG16Uint",           4,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RG16Sint,         "RG16Sint",           4,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_RG16Float,        "RG16Float",          4,   1, 1, VGPUPixelFormatKind_Float },

    { VGPUTextureFormat_RGBA8Uint,        "RGBA8Uint",          4,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RGBA8Sint,        "RGBA8Sint",          4,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_RGBA8Unorm,       "RGBA8Unorm",         4,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_RGBA8UnormSrgb,   "RGBA8UnormSrgb",     4,   1, 1, VGPUPixelFormatKind_UnormSrgb  },
    { VGPUTextureFormat_RGBA8Snorm,       "RGBA8Snorm",         4,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_BGRA8Unorm,       "BGRA8Unorm",         4,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_BGRA8UnormSrgb,   "BGRA8UnormSrgb",     4,   1, 1, VGPUPixelFormatKind_UnormSrgb },

    { VGPUTextureFormat_RGB9E5Ufloat,    "RGB9E5Ufloat",        4,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_RGB10A2Unorm,    "RGB10A2Unorm",        4,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_RGB10A2Uint,     "RGB10A2Uint",         4,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RG11B10Float,    "RG11B10Float",        4,   1, 1, VGPUPixelFormatKind_Float },

    { VGPUTextureFormat_RG32Uint,         "RG32Uint",           8,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RG32Sint,         "RG32Sint",           8,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_RG32Float,        "RG32Float",          8,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_RGBA16Unorm,      "RGBA16Unorm",        8,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_RGBA16Snorm,      "RGBA16Snorm",        8,   1, 1, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_RGBA16Uint,       "RGBA16Uint",         8,   1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RGBA16Sint,       "RGBA16Sint",         8,   1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_RGBA16Float,      "RGBA16Float",        8,   1, 1, VGPUPixelFormatKind_Float },

    { VGPUTextureFormat_RGBA32Uint,       "RGBA32Uint",        16,  1, 1, VGPUPixelFormatKind_Uint },
    { VGPUTextureFormat_RGBA32Sint,       "RGBA32Sint",        16,  1, 1, VGPUPixelFormatKind_Sint },
    { VGPUTextureFormat_RGBA32Float,      "RGBA32Float",       16,  1, 1, VGPUPixelFormatKind_Float },

    // Depth-stencil formats
    { VGPUTextureFormat_Stencil8,               "Stencil8",                 4,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_Depth16Unorm,           "Depth16Unorm",             2,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Depth32Float,           "Depth32Float",             4,   1, 1, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_Depth24UnormStencil8,   "Depth24UnormStencil8",     4,   1, 1, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Depth32FloatStencil8,   "Depth32FloatStencil8",     8,   1, 1, VGPUPixelFormatKind_Float },

    // BC compressed formats
    { VGPUTextureFormat_Bc1RgbaUnorm,       "BC1RgbaUnorm",         8,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc1RgbaUnormSrgb,   "BC1RgbaUnormSrgb",     8,   4, 4, VGPUPixelFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc2RgbaUnorm,       "BC2RgbaUnorm",         16,  4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc2RgbaUnormSrgb,   "BC2RgbaUnormSrgb",     16,  4, 4, VGPUPixelFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc3RgbaUnorm,       "BC3RgbaUnorm",         16,  4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc3RgbaUnormSrgb,   "BC3RgbaUnormSrgb",     16,  4, 4, VGPUPixelFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc4RUnorm,          "BC4RUnorm",            8,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc4RSnorm,          "BC4RSnorm",            8,   4, 4, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_Bc5RgUnorm,         "BC5Unorm",             16,  4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc5RgSnorm,         "BC5Snorm",             16,  4, 4, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_Bc6hRgbUfloat,      "Bc6hRgbUfloat",        16,  4, 4, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_Bc6hRgbSfloat,      "Bc6hRgbSfloat",        16,  4, 4, VGPUPixelFormatKind_Float },
    { VGPUTextureFormat_Bc7RgbaUnorm,       "Bc7RgbaUnorm",         16,  4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Bc7RgbaUnormSrgb,   "Bc7RgbaUnormSrgb",     16,  4, 4, VGPUPixelFormatKind_UnormSrgb },

    // ETC2/EAC compressed formats
    { VGPUTextureFormat_Etc2Rgb8Unorm,       "Etc2Rgb8Unorm",         8,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8UnormSrgb,   "Etc2Rgb8UnormSrgb",     8,   4, 4, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgb8A1Unorm,     "Etc2Rgb8A1Unorm,",     16,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8A1UnormSrgb, "Etc2Rgb8A1UnormSrgb",  16,   4, 4, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgba8Unorm,      "Etc2Rgba8Unorm",       16,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgba8UnormSrgb,  "Etc2Rgba8UnormSrgb",   16,   4, 4, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_EacR11Unorm,         "EacR11Unorm",          8,    4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_EacR11Snorm,         "EacR11Snorm",          8,    4, 4, VGPUPixelFormatKind_Snorm },
    { VGPUTextureFormat_EacRg11Unorm,        "EacRg11Unorm",         16,   4, 4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_EacRg11Snorm,        "EacRg11Snorm",         16,   4, 4, VGPUPixelFormatKind_Snorm },

    // ASTC compressed formats
    { VGPUTextureFormat_Astc4x4Unorm,        "ASTC4x4Unorm",         16,   4,   4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc4x4UnormSrgb,    "ASTC4x4UnormSrgb",     16,   4,   4, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x4Unorm,        "ASTC5x4Unorm",         16,   5,   4, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x4UnormSrgb,    "ASTC5x4UnormSrgb",     16,   5,   4, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x5Unorm,        "ASTC5x5UnormSrgb",     16,   5,   5, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x5UnormSrgb,    "ASTC5x5UnormSrgb",     16,   5,   5, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x5Unorm,        "ASTC6x5Unorm",         16,   6,   5, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x5UnormSrgb,    "ASTC6x5UnormSrgb",     16,   6,   5, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x6Unorm,        "ASTC6x6Unorm",         16,   6,   6, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x6UnormSrgb,    "ASTC6x6UnormSrgb",     16,   6,   6, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x5Unorm,        "ASTC8x5Unorm",         16,   8,   5, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x5UnormSrgb,    "ASTC8x5UnormSrgb",     16,   8,   5, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x6Unorm,        "ASTC8x6Unorm",         16,   8,   6, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x6UnormSrgb,    "ASTC8x6UnormSrgb",     16,   8,   6, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x8Unorm,        "ASTC8x8Unorm",         16,   8,   8, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x8UnormSrgb,    "ASTC8x8UnormSrgb",     16,   8,   8, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x5Unorm,       "ASTC10x5Unorm",        16,   10,  5, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x5UnormSrgb,   "ASTC10x5UnormSrgb",    16,   10,  5, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x6Unorm,       "ASTC10x6Unorm",        16,   10,  6, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x6UnormSrgb,   "ASTC10x6UnormSrgb",    16,   10,  6, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x8Unorm,       "ASTC10x8Unorm",        16,   10,  8, VGPUPixelFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x8UnormSrgb,   "ASTC10x8UnormSrgb",    16,   10,  8, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x10Unorm,      "ASTC10x10Unorm",       16,   10,  10, VGPUPixelFormatKind_Unorm  },
    { VGPUTextureFormat_Astc10x10UnormSrgb,  "ASTC10x10UnormSrgb",   16,   10,  10, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x10Unorm,      "ASTC12x10Unorm",       16,   12,  10, VGPUPixelFormatKind_Unorm  },
    { VGPUTextureFormat_Astc12x10UnormSrgb,  "ASTC12x10UnormSrgb",   16,   12,  10, VGPUPixelFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x12Unorm,      "ASTC12x12Unorm",       16,   12,  12, VGPUPixelFormatKind_Unorm  },
    { VGPUTextureFormat_Astc12x12UnormSrgb,  "ASTC12x12UnormSrgb",   16,   12,  12, VGPUPixelFormatKind_UnormSrgb },
};

VGPUBool32 vgpuIsDepthFormat(VGPUPixelFormat format)
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

VGPUBool32 vgpuIsDepthOnlyFormat(VGPUPixelFormat format)
{
    switch (format)
    {
        case VGPUTextureFormat_Depth16Unorm:
        case VGPUTextureFormat_Depth32Float:
            return true;
        default:
            return false;
    }
}

VGPUBool32 vgpuIsStencilOnlyFormat(VGPUPixelFormat format)
{
    switch (format)
    {
        case VGPUTextureFormat_Stencil8:
            return true;
        default:
            return false;
    }
}

VGPUBool32 vgpuIsStencilFormat(VGPUPixelFormat format)
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

VGPUBool32 vgpuIsDepthStencilFormat(VGPUPixelFormat format)
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

VGPUBool32 vgpuIsCompressedFormat(VGPUPixelFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);

    return c_FormatInfo[(uint32_t)format].blockWidth > 1 || c_FormatInfo[(uint32_t)format].blockHeight > 1;
}

VGPUPixelFormatKind vgpuGetPixelFormatKind(VGPUPixelFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].kind;
}


