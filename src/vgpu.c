// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgpu_driver.h"
#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#elif defined(_WIN32)
#   include <Windows.h>
#endif

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

static VGPULogCallback s_LogFunc = VGPU_DefaultLogCallback;

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

void VGPU_SetLogCallback(VGPULogCallback func)
{
    s_LogFunc = func;
}

static const VGFXDriver* drivers[] = {
#if defined(VGPU_D3D12_DRIVER)
    &D3D12_Driver,
#endif
#if defined(VGPU_D3D11_DRIVER)
    &D3D11_Driver,
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

bool vgpuIsSupported(VGPUBackendType backend)
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
    if (backend == VGPU_BACKEND_TYPE_DEFAULT)
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
                    backend = VGPU_BACKEND_TYPE_DEFAULT;
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

bool vgpuQueryFeature(VGPUDevice device, VGPUFeature feature)
{
    if (device == NULL) {
        return false;
    }

    return device->hasFeature(device->driverData, feature);
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
    def.type = _VGPU_DEF(def.type, VGPU_TEXTURE_TYPE_2D);
    def.format = _VGPU_DEF(def.type, VGFXTextureFormat_RGBA8UNorm);
    //def.usage = _VGPU_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.width = _VGPU_DEF(def.width, 1);
    def.height = _VGPU_DEF(def.height, 1);
    def.depthOrArraySize = _VGPU_DEF(def.depthOrArraySize, 1);
    def.mipLevelCount = _VGPU_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);
    return device->createTexture(device->driverData, &desc_def);
}

void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture)
{
    NULL_RETURN(device);
    NULL_RETURN(texture);

    device->destroyTexture(device->driverData, texture);
}

/* SwapChain */
static VGPUSwapChainDesc _vgpuSwapChainDescDef(const VGPUSwapChainDesc* desc)
{
    VGPUSwapChainDesc def = *desc;
    def.format = _VGPU_DEF(def.format, VGFXTextureFormat_BGRA8UNorm);
    def.presentMode = _VGPU_DEF(def.presentMode, VGPU_PRESENT_MODE_IMMEDIATE);
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

VGFXTextureFormat vgpuSwapChainGetFormat(VGPUDevice device, VGPUSwapChain swapChain)
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

void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport)
{
    VGPU_ASSERT(viewport);

    commandBuffer->setViewport(commandBuffer->driverData, viewport);
}

void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* scissorRect)
{
    VGPU_ASSERT(scissorRect);

    commandBuffer->setScissorRect(commandBuffer->driverData, scissorRect);
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
    { VGFXTextureFormat_Undefined,      "Undefined",           0,   0,  VGPU_TEXTURE_FORMAT_KIND_INTEGER,      false, false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8UInt,           "R8_UINT",           1,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8SInt,           "R8_SINT",           1,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R8UNorm,          "R8_UNORM",          1,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8SNorm,          "R8_SNORM",          1,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },

    { VGFXTextureFormat_R16UInt,          "R16_UINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16SInt,          "R16_SINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R16UNorm,         "R16_UNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16SNorm,         "R16_SNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16Float,         "R16_FLOAT",         2,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_RG8UInt,          "RG8_UINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG8SInt,          "RG8_SINT",          2,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG8UNorm,         "RG8_UNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG8SNorm,         "RG8_SNORM",         2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },

    { VGFXTextureFormat_BGRA4UNorm,       "BGRA4_UNORM",       2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_B5G6R5UNorm,      "B5G6R5_UNORM",      2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  false, false, false, false, false },
    { VGFXTextureFormat_B5G5R5A1UNorm,    "B5G5R5A1_UNORM",    2,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },

    { VGFXTextureFormat_R32UInt,          "R32_UINT",          4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R32SInt,          "R32_SINT",          4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R32Float,         "R32_FLOAT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_RG16UInt,         "RG16_UINT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16SInt,         "RG16_SINT",         4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG16UNorm,        "RG16_UNORM",        4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16SNorm,        "RG16_SNORM",        4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16Float,        "RG16_FLOAT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,        true,  true,  false, false, false, false, true,  false },

    { VGFXTextureFormat_RGBA8UInt,        "RGBA8_UINT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA8SInt,        "RGBA8_SINT",        4,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,      true,  true,  true,  true,  false, false, true,  false },
    { VGFXTextureFormat_RGBA8UNorm,       "RGBA8_UNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA8UNormSrgb,      "SRGBA8_UNORM",      4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, true  },
    { VGFXTextureFormat_RGBA8SNorm,       "RGBA8_SNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BGRA8UNorm,       "BGRA8_UNORM",       4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BGRA8UNormSrgb,      "SBGRA8_UNORM",      4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,   true,  true,  true,  true,  false, false, false, false },

    { VGFXTextureFormat_RGB10A2UNorm,   "R10G10B10A2_UNORM",    4,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,          true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RG11B10Float,   "R11G11B10_FLOAT",      4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,     true,  true,  true,  false, false, false, false, false },
    { VGFXTextureFormat_RGB9E5Float,    "RGB9E5Float",          4,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,     true,  true,  true,  false, false, false, false, false },

    { VGFXTextureFormat_RG32UInt,         "RG32_UINT",         8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG32SInt,         "RG32_SINT",         8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG32Float,        "RG32_FLOAT",        8,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RGBA16UInt,       "RGBA16_UINT",       8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16SInt,       "RGBA16_SINT",       8,   1, VGPU_TEXTURE_FORMAT_KIND_INTEGER,    true,  true,  true,  true,  false, false, true,  false },
    { VGFXTextureFormat_RGBA16UNorm,      "RGBA16_UNORM",      8,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,           true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16SNorm,      "RGBA16_SNORM",      8,   1, VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,           true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16Float,      "RGBA16_FLOAT",      8,   1, VGPU_TEXTURE_FORMAT_KIND_FLOAT,      true,  true,  true,  true,  false, false, true,  false },

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

void vgpuGetFormatInfo(VGFXTextureFormat format, const VGPUFormatInfo* pInfo)
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

bool vgpuIsDepthFormat(VGFXTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth;
}

bool vgpuIsStencilFormat(VGFXTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasStencil;
}

bool vgpuIsDepthStencilFormat(VGFXTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth || c_FormatInfo[(uint32_t)format].hasStencil;
}
