// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_WEBGPU_DRIVER)

#include "vgpu_driver.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5_webgpu.h>
#else
#error "Enable Dawn WebGPU implementation"
#endif

struct VGFXWebGPURenderer
{
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSwapChain swapchain;
};

static void webgpu_destroyDevice(VGFXDevice device)
{
    VGFXWebGPURenderer* renderer = (VGFXWebGPURenderer*)device->driverData;

    if (renderer->device)
    {
#ifndef __EMSCRIPTEN__
        wgpuSwapChainRelease(renderer->swapchain);
        wgpuQueueRelease(renderer->queue);
        wgpuDeviceRelease(renderer->device);
#endif

        renderer->device = nullptr;
    }

    delete renderer;
    VGFX_FREE(device);
}


static void webgpu_frame(VGFXRenderer* driverData)
{
    VGFXWebGPURenderer* renderer = (VGFXWebGPURenderer*)driverData;

    WGPUTextureView backbufferView = wgpuSwapChainGetCurrentTextureView(renderer->swapchain); // create textureView

    WGPURenderPassColorAttachment colorDesc = {};
    colorDesc.view = backbufferView;
    colorDesc.loadOp = WGPULoadOp_Clear;
    colorDesc.storeOp = WGPUStoreOp_Store;
    colorDesc.clearColor.r = 1.0f;
    colorDesc.clearColor.g = 0.0f;
    colorDesc.clearColor.b = 0.0f;
    colorDesc.clearColor.a = 1.0f;

    WGPURenderPassDescriptor renderPass = {};
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(renderer->device, nullptr);	// create encoder
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPass);	// create pass

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);														// release pass
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);				// create commands
    wgpuCommandEncoderRelease(encoder);														// release encoder

    wgpuQueueSubmit(renderer->queue, 1, &commands);
    wgpuCommandBufferRelease(commands);														// release commands
    wgpuSwapChainPresent(renderer->swapchain);
    // Release backbuffer View
    wgpuTextureViewRelease(backbufferView);
}

static void webgpu_waitIdle(VGFXRenderer* driverData)
{
    VGFXWebGPURenderer* renderer = (VGFXWebGPURenderer*)driverData;
    _VGFX_UNUSED(renderer);
}

static bool webgpu_queryFeature(VGFXRenderer* driverData, VGFXFeature feature)
{
    VGFXWebGPURenderer* renderer = (VGFXWebGPURenderer*)driverData;
    _VGFX_UNUSED(renderer);
    switch (feature)
    {
        case VGFX_FEATURE_COMPUTE:
            return true;

        default:
            return false;
    }
}

static bool webgpu_isSupported(void)
{
    return true;
}

static VGFXDevice webgpu_createDevice(VGFXSurface surface, const VGFXDeviceInfo* info)
{
    VGFX_ASSERT(info);
    VGFX_ASSERT(surface->type == VGFX_SURFACE_TYPE_WEB);

    VGFXWebGPURenderer* renderer = new VGFXWebGPURenderer();
#if defined(__EMSCRIPTEN__)
    renderer->device = emscripten_webgpu_get_device();
#else
#endif

    if (renderer->device == nullptr)
    {
        delete renderer;
        return nullptr;
    }

    renderer->queue = wgpuDeviceGetQueue(renderer->device);

#if defined(__EMSCRIPTEN__)
    // Create surface first.
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvDesc = {};
    canvDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvDesc.selector = surface->selector;
    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);
    WGPUSurface wgpuSurface = wgpuInstanceCreateSurface(nullptr, &surfDesc);

    WGPUSwapChainDescriptor swapDesc = {};
    swapDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
    swapDesc.width = 800;
    swapDesc.height = 450;
    swapDesc.presentMode = WGPUPresentMode_Fifo;
    renderer->swapchain = wgpuDeviceCreateSwapChain(renderer->device, wgpuSurface, &swapDesc);
#else

#endif

    vgfxLogInfo("vgfx driver: WebGPU");

    VGFXDevice_T* device = (VGFXDevice_T*)VGFX_MALLOC(sizeof(VGFXDevice_T));
    ASSIGN_DRIVER(webgpu);

    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver webgpu_driver = {
    VGFX_API_WEBGPU,
    webgpu_isSupported,
    webgpu_createDevice
};

#endif /* VGPU_WEBGPU_DRIVER */
