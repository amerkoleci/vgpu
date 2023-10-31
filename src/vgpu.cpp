// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgpu_driver.h"
#include <stdio.h>
#include <stdarg.h>

#define MAX_MESSAGE_SIZE 1024

static VGPULogLevel s_LogLevel = VGPULogLevel_Off;
static VGPULogCallback s_LogFunc = nullptr;
static void* s_userData = nullptr;

bool vgpuShouldLog(VGPULogLevel level)
{
    if (s_LogLevel == VGPULogLevel_Off || s_LogFunc == nullptr)
        return false;

    return level <= s_LogLevel;
}

void vgpuLogInfo(const char* format, ...)
{
    if (!vgpuShouldLog(VGPULogLevel_Info))
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);

    s_LogFunc(VGPULogLevel_Info, msg, s_userData);
}

void vgpuLogWarn(const char* format, ...)
{
    if (!vgpuShouldLog(VGPULogLevel_Warn))
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPULogLevel_Warn, msg, s_userData);
}

void vgpuLogError(const char* format, ...)
{
    if (!vgpuShouldLog(VGPULogLevel_Error))
        return;

    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);

    s_LogFunc(VGPULogLevel_Error, msg, s_userData);
}

VGPULogLevel vgpuGetLogLevel(void)
{
    return s_LogLevel;
}

void vgpuSetLogLevel(VGPULogLevel level)
{
    s_LogLevel = level;
}

void vgpuSetLogCallback(VGPULogCallback func, void* userData)
{
    s_LogFunc = func;
    s_userData = userData;
}

static const VGPUDriver* drivers[] = {
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
            return drivers[i]->isSupported();
        }
    }

    return false;
}

VGPUDevice vgpuCreateDevice(const VGPUDeviceDescriptor* desc)
{
    if (!desc) {
        vgpuLogWarn("vgpu_init: Invalid config");
        return nullptr;
    }

    VGPUDevice device = NULL;
    VGPUBackend backend = desc->preferredBackend;

retry:
    if (backend == _VGPUBackend_Default)
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->isSupported())
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
                if (drivers[i]->isSupported())
                {
                    device = drivers[i]->createDevice(desc);
                    break;
                }
                else
                {
                    vgpuLogWarn("Wanted API not supported, fallback to default");
                    backend = _VGPUBackend_Default;
                    goto retry;
                }
            }
        }
    }

    return device;
}

void vgpuDeviceSetLabel(VGPUDevice device, const char* label)
{
    NULL_RETURN(device);

    device->SetLabel(label);
}

uint32_t vgpuDeviceAddRef(VGPUDevice device)
{
    VGPU_ASSERT(device);

    return device->AddRef();
}

uint32_t vgpuDeviceRelease(VGPUDevice device)
{
    VGPU_ASSERT(device);

    return device->Release();
}

void vgpuDeviceWaitIdle(VGPUDevice device)
{
    device->WaitIdle();
}

VGPUBackend vgpuDeviceGetBackend(VGPUDevice device)
{
    return device->GetBackendType();
}

VGPUBool32 vgpuDeviceQueryFeatureSupport(VGPUDevice device, VGPUFeature feature)
{
    if (device == NULL) {
        return false;
    }

    return device->QueryFeatureSupport(feature);
}

void vgpuDeviceGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties)
{
    VGPU_ASSERT(device);
    NULL_RETURN(properties);

    device->GetAdapterProperties(properties);
}

void vgpuDeviceGetLimits(VGPUDevice device, VGPULimits* limits)
{
    VGPU_ASSERT(device);
    NULL_RETURN(limits);

    device->GetLimits(limits);
}

uint64_t vgpuDeviceSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(commandBuffers);
    VGPU_ASSERT(count);

    return device->Submit(commandBuffers, count);
}

uint64_t vgpuDeviceGetFrameCount(VGPUDevice device)
{
    return device->GetFrameCount();
}

uint32_t vgpuDeviceGetFrameIndex(VGPUDevice device)
{
    return device->GetFrameIndex();
}

uint64_t vgpuDeviceGetTimestampFrequency(VGPUDevice device)
{
    VGPU_ASSERT(device);

    return device->GetTimestampFrequency();
}

void* vgpuDeviceGetNativeObject(VGPUDevice device, VGPUNativeObjectType objectType)
{
    return device->GetNativeObject(objectType);
}

/* Buffer */
static VGPUBufferDesc _vgpu_buffer_desc_def(const VGPUBufferDesc* desc)
{
    VGPUBufferDesc def = *desc;
    def.size = _VGPU_DEF(def.size, 4);
    return def;
}

VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDesc* desc, const void* pInitialData)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUBufferDesc desc_def = _vgpu_buffer_desc_def(desc);
    return device->CreateBuffer(&desc_def, pInitialData);
}

uint64_t vgpuBufferGetSize(VGPUBuffer buffer)
{
    VGPU_ASSERT(buffer);

    return buffer->GetSize();
}

VGPUBufferUsageFlags vgpuBufferGetUsage(VGPUBuffer buffer)
{
    VGPU_ASSERT(buffer);

    return buffer->GetUsage();
}

VGPUDeviceAddress vgpuBufferGetAddress(VGPUBuffer buffer)
{
    VGPU_ASSERT(buffer);

    return buffer->GetGpuAddress();
}

void vgpuBufferSetLabel(VGPUBuffer buffer, const char* label)
{
    NULL_RETURN(buffer);

    buffer->SetLabel(label);
}

uint32_t vgpuBufferAddRef(VGPUBuffer buffer)
{
    assert(buffer);

    return buffer->AddRef();
}

uint32_t vgpuBufferRelease(VGPUBuffer buffer)
{
    assert(buffer);

    return buffer->Release();
}

/* Texture */
static VGPUTextureDesc _vgpuTextureDescDef(const VGPUTextureDesc* desc)
{
    VGPUTextureDesc def = *desc;
    def.dimension = _VGPU_DEF(def.dimension, VGPUTextureDimension_2D);
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_RGBA8Unorm);
    //def.usage = _VGPU_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.width = _VGPU_DEF(def.width, 1);
    def.height = _VGPU_DEF(def.height, 1);
    def.depthOrArrayLayers = _VGPU_DEF(def.depthOrArrayLayers, 1);
    def.mipLevelCount = _VGPU_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* pInitialData)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);

    VGPU_ASSERT(desc_def.width > 0 && desc_def.height > 0 && desc_def.mipLevelCount >= 0);

    const bool is3D = desc_def.dimension == VGPUTextureDimension_3D;
    const bool isDepthStencil = vgpuIsDepthStencilFormat(desc_def.format);
    bool isCube = false;

    if (desc_def.dimension == VGPUTextureDimension_2D &&
        desc_def.width == desc_def.height &&
        desc_def.depthOrArrayLayers >= 6)
    {
        isCube = true;
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

        if (desc_def.mipLevelCount > 1)
        {
            vgpuLogWarn("Multisample texture cannot have mipmaps");
            return NULL;
        }
    }

    if (isDepthStencil && desc_def.mipLevelCount > 1)
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

    return device->CreateTexture(&desc_def, pInitialData);
}

VGPUTextureDimension vgpuTextureGetDimension(VGPUTexture texture)
{
    VGPU_ASSERT(texture);

    return texture->GetDimension();
}

VGPUTextureFormat vgpuTextureGetFormat(VGPUTexture texture)
{
    VGPU_ASSERT(texture);

    return texture->GetFormat();
}

void vgpuTextureSetLabel(VGPUTexture texture, const char* label)
{
    NULL_RETURN(texture);

    texture->SetLabel(label);
}

uint32_t vgpuTextureAddRef(VGPUTexture texture)
{
    assert(texture);

    return texture->AddRef();
}

uint32_t vgpuTextureRelease(VGPUTexture texture)
{
    assert(texture);

    return texture->Release();
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
    return device->CreateSampler(&desc_def);
}

void vgpuSamplerSetLabel(VGPUSampler sampler, const char* label)
{
    NULL_RETURN(sampler);

    sampler->SetLabel(label);
}

uint32_t vgpuSamplerAddRef(VGPUSampler sampler)
{
    assert(sampler);

    return sampler->AddRef();
}

uint32_t vgpuSamplerRelease(VGPUSampler sampler)
{
    assert(sampler);

    return sampler->Release();
}

/* BindGroupLayout */
static VGPUBindGroupLayoutDesc _VGPUBindGroupLayoutDesc_Def(const VGPUBindGroupLayoutDesc* desc)
{
    VGPUBindGroupLayoutDesc def = *desc;
    return def;
}

VGPUBindGroupLayout vgpuCreateBindGroupLayout(VGPUDevice device, const VGPUBindGroupLayoutDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUBindGroupLayoutDesc desc_def = _VGPUBindGroupLayoutDesc_Def(desc);
    return device->CreateBindGroupLayout(&desc_def);
}

void vgpuBindGroupLayoutSetLabel(VGPUBindGroupLayout bindGroupLayout, const char* label)
{
    VGPU_ASSERT(bindGroupLayout);

    bindGroupLayout->SetLabel(label);
}

uint32_t vgpuBindGroupLayoutAddRef(VGPUPipelineLayout bindGroupLayout)
{
    VGPU_ASSERT(bindGroupLayout);

    return bindGroupLayout->AddRef();
}

uint32_t vgpuBindGroupLayoutRelease(VGPUPipelineLayout bindGroupLayout)
{
    VGPU_ASSERT(bindGroupLayout);

    return bindGroupLayout->Release();
}

/* PipelineLayout */
static VGPUPipelineLayoutDesc _VGPUPipelineLayoutDesc_Def(const VGPUPipelineLayoutDesc* desc)
{
    VGPUPipelineLayoutDesc def = *desc;
    return def;
}

VGPUPipelineLayout vgpuCreatePipelineLayout(VGPUDevice device, const VGPUPipelineLayoutDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    VGPUPipelineLayoutDesc desc_def = _VGPUPipelineLayoutDesc_Def(desc);
    return device->CreatePipelineLayout(&desc_def);
}

void vgpuPipelineLayoutSetLabel(VGPUPipelineLayout pipelineLayout, const char* label)
{
    VGPU_ASSERT(pipelineLayout);

    pipelineLayout->SetLabel(label);
}

uint32_t vgpuPipelineLayoutAddRef(VGPUPipelineLayout pipelineLayout)
{
    VGPU_ASSERT(pipelineLayout);

    return pipelineLayout->AddRef();
}

uint32_t vgpuPipelineLayoutRelease(VGPUPipelineLayout pipelineLayout)
{
    VGPU_ASSERT(pipelineLayout);

    return pipelineLayout->Release();
}

/* Pipeline */
static VGPURenderPipelineDesc _vgpuRenderPipelineDescDef(const VGPURenderPipelineDesc* desc)
{
    VGPURenderPipelineDesc def = *desc;

    // BlendState
    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; ++i)
    {
        def.blendState.renderTargets[i].srcColorBlendFactor = _VGPU_DEF(def.blendState.renderTargets[0].srcColorBlendFactor, VGPUBlendFactor_One);
        def.blendState.renderTargets[i].dstColorBlendFactor = _VGPU_DEF(def.blendState.renderTargets[0].dstColorBlendFactor, VGPUBlendFactor_Zero);
        def.blendState.renderTargets[i].colorBlendOperation = _VGPU_DEF(def.blendState.renderTargets[0].colorBlendOperation, VGPUBlendOperation_Add);
        def.blendState.renderTargets[i].srcAlphaBlendFactor = _VGPU_DEF(def.blendState.renderTargets[0].srcAlphaBlendFactor, VGPUBlendFactor_One);
        def.blendState.renderTargets[i].dstAlphaBlendFactor = _VGPU_DEF(def.blendState.renderTargets[0].dstAlphaBlendFactor, VGPUBlendFactor_Zero);
        def.blendState.renderTargets[i].alphaBlendOperation = _VGPU_DEF(def.blendState.renderTargets[0].alphaBlendOperation, VGPUBlendOperation_Add);
        def.blendState.renderTargets[i].colorWriteMask = _VGPU_DEF(def.blendState.renderTargets[0].colorWriteMask, VGPUColorWriteMask_All);
    }

    // DepthStencilState
    def.depthStencilState.depthCompareFunction = _VGPU_DEF(def.depthStencilState.depthCompareFunction, VGPUCompareFunction_Always);
    def.depthStencilState.stencilFront.compareFunction = _VGPU_DEF(def.depthStencilState.stencilFront.compareFunction, VGPUCompareFunction_Always);
    def.depthStencilState.stencilFront.failOperation = _VGPU_DEF(def.depthStencilState.stencilFront.failOperation, VGPUStencilOperation_Keep);
    def.depthStencilState.stencilFront.depthFailOperation = _VGPU_DEF(def.depthStencilState.stencilFront.depthFailOperation, VGPUStencilOperation_Keep);
    def.depthStencilState.stencilFront.passOperation = _VGPU_DEF(def.depthStencilState.stencilFront.passOperation, VGPUStencilOperation_Keep);

    def.depthStencilState.stencilBack.compareFunction = _VGPU_DEF(def.depthStencilState.stencilBack.compareFunction, VGPUCompareFunction_Always);
    def.depthStencilState.stencilBack.failOperation = _VGPU_DEF(def.depthStencilState.stencilBack.failOperation, VGPUStencilOperation_Keep);
    def.depthStencilState.stencilBack.depthFailOperation = _VGPU_DEF(def.depthStencilState.stencilBack.depthFailOperation, VGPUStencilOperation_Keep);
    def.depthStencilState.stencilBack.passOperation = _VGPU_DEF(def.depthStencilState.stencilBack.passOperation, VGPUStencilOperation_Keep);

    def.primitiveTopology = _VGPU_DEF(def.primitiveTopology, VGPUPrimitiveTopology_TriangleList);
    def.patchControlPoints = _VGPU_DEF(def.patchControlPoints, 1);

    //def.depthStencilState.stencilReadMask = _VGPU_DEF(def.depthStencilState.stencilReadMask, 0xFF);
    //def.depthStencilState.stencilWriteMask = _VGPU_DEF(def.depthStencilState.stencilWriteMask, 0xFF);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->layout);
    VGPU_ASSERT(desc->shaderStageCount > 0);
    VGPU_ASSERT(desc->shaderStages != nullptr);

    VGPURenderPipelineDesc desc_def = _vgpuRenderPipelineDescDef(desc);
    return device->CreateRenderPipeline(&desc_def);
}

VGPUPipeline vgpuCreateComputePipeline(VGPUDevice device, const VGPUComputePipelineDesc* desc)
{
    VGPU_ASSERT(device);

    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->layout);
    VGPU_ASSERT(desc->computeShader.stage == VGPUShaderStage_Compute);
    VGPU_ASSERT(desc->computeShader.bytecode != nullptr);
    VGPU_ASSERT(desc->computeShader.size > 0);

    return device->CreateComputePipeline(desc);
}

VGPUPipeline vgpuCreateRayTracingPipeline(VGPUDevice device, const VGPURayTracingPipelineDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    return device->CreateRayTracingPipeline(desc);
}

VGPUPipelineType vgpuPipelineGetType(VGPUPipeline pipeline)
{
    VGPU_ASSERT(pipeline);

    return pipeline->GetType();
}

void vgpuPipelineSetLabel(VGPUPipeline pipeline, const char* label)
{
    NULL_RETURN(pipeline);

    pipeline->SetLabel(label);
}

uint32_t vgpuPipelineAddRef(VGPUPipeline pipeline)
{
    VGPU_ASSERT(pipeline);

    return pipeline->AddRef();
}

uint32_t vgpuPipelineRelease(VGPUPipeline pipeline)
{
    VGPU_ASSERT(pipeline);

    return pipeline->Release();
}

/* QueryHeap */
VGPUQueryHeap vgpuCreateQueryHeap(VGPUDevice device, const VGPUQueryHeapDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(desc);

    return device->CreateQueryHeap(desc);
}

VGPUQueryType vgpuQueryHeapGetType(VGPUQueryHeap queryHeap)
{
    VGPU_ASSERT(queryHeap);

    return queryHeap->GetType();
}

uint32_t vgpuQuerySetGetCount(VGPUQueryHeap queryHeap)
{
    VGPU_ASSERT(queryHeap);

    return queryHeap->GetCount();
}

void vgpuQueryHeapSetLabel(VGPUQueryHeap queryHeap, const char* label)
{
    NULL_RETURN(queryHeap);

    queryHeap->SetLabel(label);
}

uint32_t vgpuQueryHeapAddRef(VGPUQueryHeap queryHeap)
{
    VGPU_ASSERT(queryHeap);

    return queryHeap->AddRef();
}

uint32_t vgpuQueryHeapRelease(VGPUQueryHeap queryHeap)
{
    VGPU_ASSERT(queryHeap);

    return queryHeap->Release();
}

/* SwapChain */
static VGPUSwapChainDesc _vgpuSwapChainDescDef(const VGPUSwapChainDesc* desc)
{
    VGPUSwapChainDesc def = *desc;
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_BGRA8Unorm);
    def.presentMode = _VGPU_DEF(def.presentMode, VGPUPresentMode_Immediate);
    return def;
}

VGPUSwapChain vgpuCreateSwapChain(VGPUDevice device, void* window, const VGPUSwapChainDesc* desc)
{
    VGPU_ASSERT(device);
    NULL_RETURN_NULL(window);
    NULL_RETURN_NULL(desc);

    VGPUSwapChainDesc def = _vgpuSwapChainDescDef(desc);
    return device->CreateSwapChain(window, &def);
}

VGPUTextureFormat vgpuSwapChainGetFormat(VGPUSwapChain swapChain)
{
    VGPU_ASSERT(swapChain);

    return swapChain->GetFormat();
}

void vgpuSwapChainGetSize(VGPUSwapChain swapChain, uint32_t* width, uint32_t* height)
{
    VGPU_ASSERT(swapChain);

    if (width)
        *width = swapChain->GetWidth();
    if (height)
        *height = swapChain->GetHeight();

}

uint32_t vgpuSwapChainAddRef(VGPUSwapChain swapChain)
{
    VGPU_ASSERT(swapChain);

    return swapChain->AddRef();
}

uint32_t vgpuSwapChainRelease(VGPUSwapChain swapChain)
{
    VGPU_ASSERT(swapChain);

    return swapChain->Release();
}

/* Commands */
VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, VGPUCommandQueue queueType, const char* label)
{
    VGPU_ASSERT(device);

    return device->BeginCommandBuffer(queueType, label);
}

void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel)
{
    NULL_RETURN(groupLabel);

    commandBuffer->PushDebugGroup(groupLabel);
}

void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer)
{
    commandBuffer->PopDebugGroup();
}

void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* markerLabel)
{
    NULL_RETURN(markerLabel);

    commandBuffer->InsertDebugMarker(markerLabel);
}

void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline pipeline)
{
    VGPU_ASSERT(pipeline);

    commandBuffer->SetPipeline(pipeline);
}

void vgpuSetPushConstants(VGPUCommandBuffer commandBuffer, uint32_t pushConstantIndex, const void* data, uint32_t size)
{
    VGPU_ASSERT(data);
    VGPU_ASSERT(size);

    commandBuffer->SetPushConstants(pushConstantIndex, data, size);
}

void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    commandBuffer->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void vgpuDispatch1D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t groupSizeX)
{
    commandBuffer->Dispatch(DivideByMultiple(threadCountX, groupSizeX), 1, 1);
}

void vgpuDispatch2D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY)
{
    commandBuffer->Dispatch(
        DivideByMultiple(threadCountX, groupSizeX),
        DivideByMultiple(threadCountY, groupSizeY), 1);
}

void vgpuDispatch3D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
    commandBuffer->Dispatch(
        DivideByMultiple(threadCountX, groupSizeX),
        DivideByMultiple(threadCountY, groupSizeY),
        DivideByMultiple(threadCountZ, groupSizeZ)
    );
}

void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset)
{
    NULL_RETURN(buffer);

    commandBuffer->DispatchIndirect(buffer, offset);
}

VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain swapChain)
{
    return commandBuffer->AcquireSwapchainTexture(swapChain);
}

void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc)
{
    NULL_RETURN(desc);

    commandBuffer->BeginRenderPass(desc);
}

void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer)
{
    commandBuffer->EndRenderPass();
}

void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport)
{
    VGPU_ASSERT(viewport);

    commandBuffer->SetViewport(viewport);
}

void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports)
{
    VGPU_ASSERT(count > 0);
    VGPU_ASSERT(viewports);

    commandBuffer->SetViewports(count, viewports);
}

void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* scissorRect)
{
    VGPU_ASSERT(scissorRect);

    commandBuffer->SetScissorRect(scissorRect);
}

void vgpuSetScissorRects(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPURect* scissorRects)
{
    VGPU_ASSERT(count);
    VGPU_ASSERT(scissorRects);

    commandBuffer->SetScissorRects(count, scissorRects);
}

void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, VGPUBuffer buffer, uint64_t offset)
{
    commandBuffer->SetVertexBuffer(index, buffer, offset);
}

void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, VGPUIndexType type, uint64_t offset)
{
    commandBuffer->SetIndexBuffer(buffer, type, offset);
}

void vgpuSetStencilReference(VGPUCommandBuffer commandBuffer, uint32_t reference)
{
    commandBuffer->SetStencilReference(reference);
}

void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    commandBuffer->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void vgpuDrawIndexed(VGPUCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    commandBuffer->DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void vgpuDrawIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset)
{
    commandBuffer->DrawIndirect(indirectBuffer, indirectBufferOffset);
}

void vgpuDrawIndexedIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset)
{
    commandBuffer->DrawIndexedIndirect(indirectBuffer, indirectBufferOffset);
}

void vgpuBeginQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index)
{
    commandBuffer->BeginQuery(queryHeap, index);
}

void vgpuEndQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index)
{
    commandBuffer->EndQuery(queryHeap, index);
}

void vgpuResolveQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset)
{
    commandBuffer->ResolveQuery(queryHeap, index, count, destinationBuffer, destinationOffset);
}

void vgpuResetQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index, uint32_t count)
{
    commandBuffer->ResetQuery(queryHeap, index, count);
}

// Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
static const VGPUPixelFormatInfo c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
    { VGPUTextureFormat_Undefined,      "Undefined",              0,   0, 0,  _VGPUFormatKind_Force32 },
    { VGPUTextureFormat_R8Unorm,        "R8Unorm",              1,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_R8Snorm,        "R8Snorm",              1,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_R8Uint,         "R8Uint",               1,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_R8Sint,         "R8USnt",               1,   1, 1, VGPUFormatKind_Sint },

    { VGPUTextureFormat_R16Unorm,         "R16Unorm",           2,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_R16Snorm,         "R16Snorm",           2,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_R16Uint,          "R16Uint",            2,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_R16Sint,          "R16Sint",            2,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_R16Float,         "R16Float",           2,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_RG8Unorm,         "RG8Unorm",           2,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_RG8Snorm,         "RG8Snorm",           2,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_RG8Uint,          "RG8Uint",            2,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RG8Sint,          "RG8Sint",            2,   1, 1, VGPUFormatKind_Sint },

    { VGPUTextureFormat_BGRA4Unorm,       "BGRA4Unorm",         2,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_B5G6R5Unorm,      "B5G6R5Unorm",        2,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_B5G5R5A1Unorm,    "B5G5R5A1Unorm",      2,   1, 1, VGPUFormatKind_Unorm },

    { VGPUTextureFormat_R32Uint,          "R32Uint",            4,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_R32Sint,          "R32Sint",            4,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_R32Float,         "R32Float",           4,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_RG16Unorm,        "RG16Unorm",          4,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_RG16Snorm,        "RG16Snorm",          4,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_RG16Uint,         "RG16Uint",           4,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RG16Sint,         "RG16Sint",           4,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_RG16Float,        "RG16Float",          4,   1, 1, VGPUFormatKind_Float },

    { VGPUTextureFormat_RGBA8Uint,        "RGBA8Uint",          4,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RGBA8Sint,        "RGBA8Sint",          4,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_RGBA8Unorm,       "RGBA8Unorm",         4,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_RGBA8UnormSrgb,   "RGBA8UnormSrgb",     4,   1, 1, VGPUFormatKind_UnormSrgb  },
    { VGPUTextureFormat_RGBA8Snorm,       "RGBA8Snorm",         4,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_BGRA8Unorm,       "BGRA8Unorm",         4,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_BGRA8UnormSrgb,   "BGRA8UnormSrgb",     4,   1, 1, VGPUFormatKind_UnormSrgb },

    { VGPUTextureFormat_RGB9E5Ufloat,    "RGB9E5Ufloat",        4,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_RGB10A2Unorm,    "RGB10A2Unorm",        4,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_RGB10A2Uint,     "RGB10A2Uint",         4,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RG11B10Float,    "RG11B10Float",        4,   1, 1, VGPUFormatKind_Float },

    { VGPUTextureFormat_RG32Uint,         "RG32Uint",           8,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RG32Sint,         "RG32Sint",           8,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_RG32Float,        "RG32Float",          8,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_RGBA16Unorm,      "RGBA16Unorm",        8,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_RGBA16Snorm,      "RGBA16Snorm",        8,   1, 1, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_RGBA16Uint,       "RGBA16Uint",         8,   1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RGBA16Sint,       "RGBA16Sint",         8,   1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_RGBA16Float,      "RGBA16Float",        8,   1, 1, VGPUFormatKind_Float },

    { VGPUTextureFormat_RGBA32Uint,       "RGBA32Uint",        16,  1, 1, VGPUFormatKind_Uint },
    { VGPUTextureFormat_RGBA32Sint,       "RGBA32Sint",        16,  1, 1, VGPUFormatKind_Sint },
    { VGPUTextureFormat_RGBA32Float,      "RGBA32Float",       16,  1, 1, VGPUFormatKind_Float },

    // Depth-stencil formats
    { VGPUTextureFormat_Stencil8,               "Stencil8",                 4,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_Depth16Unorm,           "Depth16Unorm",             2,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Depth32Float,           "Depth32Float",             4,   1, 1, VGPUFormatKind_Float },
    { VGPUTextureFormat_Depth24UnormStencil8,   "Depth24UnormStencil8",     4,   1, 1, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Depth32FloatStencil8,   "Depth32FloatStencil8",     8,   1, 1, VGPUFormatKind_Float },

    // BC compressed formats
    { VGPUTextureFormat_Bc1RgbaUnorm,       "BC1RgbaUnorm",         8,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc1RgbaUnormSrgb,   "BC1RgbaUnormSrgb",     8,   4, 4, VGPUFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc2RgbaUnorm,       "BC2RgbaUnorm",         16,  4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc2RgbaUnormSrgb,   "BC2RgbaUnormSrgb",     16,  4, 4, VGPUFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc3RgbaUnorm,       "BC3RgbaUnorm",         16,  4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc3RgbaUnormSrgb,   "BC3RgbaUnormSrgb",     16,  4, 4, VGPUFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc4RUnorm,          "BC4RUnorm",            8,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc4RSnorm,          "BC4RSnorm",            8,   4, 4, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_Bc5RgUnorm,         "BC5Unorm",             16,  4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc5RgSnorm,         "BC5Snorm",             16,  4, 4, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_Bc6hRgbUfloat,      "Bc6hRgbUfloat",        16,  4, 4, VGPUFormatKind_Float },
    { VGPUTextureFormat_Bc6hRgbSfloat,      "Bc6hRgbSfloat",        16,  4, 4, VGPUFormatKind_Float },
    { VGPUTextureFormat_Bc7RgbaUnorm,       "Bc7RgbaUnorm",         16,  4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Bc7RgbaUnormSrgb,   "Bc7RgbaUnormSrgb",     16,  4, 4, VGPUFormatKind_UnormSrgb },

    // ETC2/EAC compressed formats
    { VGPUTextureFormat_Etc2Rgb8Unorm,       "Etc2Rgb8Unorm",         8,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8UnormSrgb,   "Etc2Rgb8UnormSrgb",     8,   4, 4, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgb8A1Unorm,     "Etc2Rgb8A1Unorm,",     16,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8A1UnormSrgb, "Etc2Rgb8A1UnormSrgb",  16,   4, 4, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgba8Unorm,      "Etc2Rgba8Unorm",       16,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgba8UnormSrgb,  "Etc2Rgba8UnormSrgb",   16,   4, 4, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_EacR11Unorm,         "EacR11Unorm",          8,    4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_EacR11Snorm,         "EacR11Snorm",          8,    4, 4, VGPUFormatKind_Snorm },
    { VGPUTextureFormat_EacRg11Unorm,        "EacRg11Unorm",         16,   4, 4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_EacRg11Snorm,        "EacRg11Snorm",         16,   4, 4, VGPUFormatKind_Snorm },

    // ASTC compressed formats
    { VGPUTextureFormat_Astc4x4Unorm,        "ASTC4x4Unorm",         16,   4,   4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc4x4UnormSrgb,    "ASTC4x4UnormSrgb",     16,   4,   4, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x4Unorm,        "ASTC5x4Unorm",         16,   5,   4, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x4UnormSrgb,    "ASTC5x4UnormSrgb",     16,   5,   4, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x5Unorm,        "ASTC5x5UnormSrgb",     16,   5,   5, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x5UnormSrgb,    "ASTC5x5UnormSrgb",     16,   5,   5, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x5Unorm,        "ASTC6x5Unorm",         16,   6,   5, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x5UnormSrgb,    "ASTC6x5UnormSrgb",     16,   6,   5, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x6Unorm,        "ASTC6x6Unorm",         16,   6,   6, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x6UnormSrgb,    "ASTC6x6UnormSrgb",     16,   6,   6, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x5Unorm,        "ASTC8x5Unorm",         16,   8,   5, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x5UnormSrgb,    "ASTC8x5UnormSrgb",     16,   8,   5, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x6Unorm,        "ASTC8x6Unorm",         16,   8,   6, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x6UnormSrgb,    "ASTC8x6UnormSrgb",     16,   8,   6, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x8Unorm,        "ASTC8x8Unorm",         16,   8,   8, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x8UnormSrgb,    "ASTC8x8UnormSrgb",     16,   8,   8, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x5Unorm,       "ASTC10x5Unorm",        16,   10,  5, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x5UnormSrgb,   "ASTC10x5UnormSrgb",    16,   10,  5, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x6Unorm,       "ASTC10x6Unorm",        16,   10,  6, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x6UnormSrgb,   "ASTC10x6UnormSrgb",    16,   10,  6, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x8Unorm,       "ASTC10x8Unorm",        16,   10,  8, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x8UnormSrgb,   "ASTC10x8UnormSrgb",    16,   10,  8, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x10Unorm,      "ASTC10x10Unorm",       16,   10,  10, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x10UnormSrgb,  "ASTC10x10UnormSrgb",   16,   10,  10, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x10Unorm,      "ASTC12x10Unorm",       16,   12,  10, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc12x10UnormSrgb,  "ASTC12x10UnormSrgb",   16,   12,  10, VGPUFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x12Unorm,      "ASTC12x12Unorm",       16,   12,  12, VGPUFormatKind_Unorm },
    { VGPUTextureFormat_Astc12x12UnormSrgb,  "ASTC12x12UnormSrgb",   16,   12,  12, VGPUFormatKind_UnormSrgb },
};

static const VGPUVertexFormatInfo s_VertexFormatTable[] = {
    {VGPUVertexFormat_Undefined,           0, 0, 0, _VGPUFormatKind_Count},
    {VGPUVertexFormat_UByte2,              2, 2, 1, VGPUFormatKind_Uint},
    {VGPUVertexFormat_UByte4,              4, 4, 1, VGPUFormatKind_Uint},
    {VGPUVertexFormat_Byte2,               2, 2, 1, VGPUFormatKind_Sint},
    {VGPUVertexFormat_Byte4,               4, 4, 1, VGPUFormatKind_Sint},
    {VGPUVertexFormat_UByte2Normalized,    2, 2, 1, VGPUFormatKind_Unorm},
    {VGPUVertexFormat_UByte4Normalized,    4, 4, 1, VGPUFormatKind_Unorm},
    {VGPUVertexFormat_Byte2Normalized,     2, 2, 1, VGPUFormatKind_Snorm},
    {VGPUVertexFormat_Byte4Normalized,     4, 4, 1, VGPUFormatKind_Snorm},

    {VGPUVertexFormat_UShort2,             4, 2, 2, VGPUFormatKind_Uint},
    {VGPUVertexFormat_UShort4,             8, 4, 2, VGPUFormatKind_Uint},
    {VGPUVertexFormat_Short2,              4, 2, 2, VGPUFormatKind_Sint},
    {VGPUVertexFormat_Short4,              8, 4, 2, VGPUFormatKind_Sint},
    {VGPUVertexFormat_UShort2Normalized,   4, 2, 2, VGPUFormatKind_Unorm},
    {VGPUVertexFormat_UShort4Normalized,   8, 4, 2, VGPUFormatKind_Unorm},
    {VGPUVertexFormat_Short2Normalized,    4, 2, 2, VGPUFormatKind_Snorm},
    {VGPUVertexFormat_Short4Normalized,    8, 4, 2, VGPUFormatKind_Snorm},

    {VGPUVertexFormat_Half2,               4, 2, 2, VGPUFormatKind_Float},
    {VGPUVertexFormat_Half4,               8, 4, 2, VGPUFormatKind_Float},
    {VGPUVertexFormat_Float,               4, 1, 4, VGPUFormatKind_Float},
    {VGPUVertexFormat_Float2,              8, 2, 4, VGPUFormatKind_Float},
    {VGPUVertexFormat_Float3,              12, 3, 4, VGPUFormatKind_Float},
    {VGPUVertexFormat_Float4,              16, 4, 4, VGPUFormatKind_Float},

    {VGPUVertexFormat_UInt,                4, 1, 4, VGPUFormatKind_Uint},
    {VGPUVertexFormat_UInt2,               8, 2, 4, VGPUFormatKind_Uint},
    {VGPUVertexFormat_UInt3,               12, 3, 4, VGPUFormatKind_Uint},
    {VGPUVertexFormat_UInt4,               16, 4, 4, VGPUFormatKind_Uint},

    {VGPUVertexFormat_Int,                 4, 1, 4, VGPUFormatKind_Sint},
    {VGPUVertexFormat_Int2,                8, 2, 4, VGPUFormatKind_Sint},
    {VGPUVertexFormat_Int3,                12, 3, 4, VGPUFormatKind_Sint},
    {VGPUVertexFormat_Int4,                16, 4, 4, VGPUFormatKind_Sint},

    {VGPUVertexFormat_Int1010102Normalized,    32, 4, 4, VGPUFormatKind_Unorm},
    {VGPUVertexFormat_UInt1010102Normalized,   32, 4, 4, VGPUFormatKind_Uint},
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

VGPUBool32 vgpuIsDepthOnlyFormat(VGPUTextureFormat format)
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

VGPUBool32 vgpuIsStencilOnlyFormat(VGPUTextureFormat format)
{
    switch (format)
    {
        case VGPUTextureFormat_Stencil8:
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

VGPUBool32 vgpuIsCompressedFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);

    return c_FormatInfo[(uint32_t)format].blockWidth > 1 || c_FormatInfo[(uint32_t)format].blockHeight > 1;
}

VGPUFormatKind vgpuGetPixelFormatKind(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].kind;
}

VGPUBool32 vgpuStencilTestEnabled(const VGPUDepthStencilState* depthStencil)
{
    VGPU_ASSERT(depthStencil);

    return
        depthStencil->stencilBack.compareFunction != VGPUCompareFunction_Always ||
        depthStencil->stencilBack.failOperation != VGPUStencilOperation_Keep ||
        depthStencil->stencilBack.depthFailOperation != VGPUStencilOperation_Keep ||
        depthStencil->stencilBack.passOperation != VGPUStencilOperation_Keep ||
        depthStencil->stencilFront.compareFunction != VGPUCompareFunction_Always ||
        depthStencil->stencilFront.failOperation != VGPUStencilOperation_Keep ||
        depthStencil->stencilFront.depthFailOperation != VGPUStencilOperation_Keep ||
        depthStencil->stencilFront.passOperation != VGPUStencilOperation_Keep;
}

