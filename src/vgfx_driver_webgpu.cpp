// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGFX_WEBGPU_DRIVER)

#include "vgfx_driver.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5_webgpu.h>
#else
#error "Enable Dawn WebGPU implementation"
#endif
namespace
{
}

struct gfxWebGPURenderer
{
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSwapChain swapchain;
};

static void webgpu_destroyDevice(gfxDevice device)
{
    gfxWebGPURenderer* renderer = (gfxWebGPURenderer*)device->driverData;

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


static void webgpu_frame(gfxRenderer* driverData)
{
    gfxWebGPURenderer* renderer = (gfxWebGPURenderer*)driverData;

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

    wgpuRenderPassEncoderEndPass(pass);
    wgpuRenderPassEncoderRelease(pass);														// release pass
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);				// create commands
    wgpuCommandEncoderRelease(encoder);														// release encoder

    wgpuQueueSubmit(renderer->queue, 1, &commands);
    wgpuCommandBufferRelease(commands);														// release commands
#ifndef __EMSCRIPTEN__
    /// TODO: wgpuSwapChainPresent is unsupported in Emscripten
    wgpuSwapChainPresent(renderer->swapchain);
#endif
    // Release backbuffer View
    wgpuTextureViewRelease(backbufferView);
}

static gfxDevice webgpu_createDevice(const VGFXDeviceInfo* info)
{
    gfxWebGPURenderer* renderer = new gfxWebGPURenderer();
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
    canvDesc.selector = "canvas";
    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);
    WGPUSurface surface = wgpuInstanceCreateSurface(nullptr, &surfDesc);

    WGPUSwapChainDescriptor swapDesc = {};
    swapDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
    swapDesc.width = 800;
    swapDesc.height = 450;
    swapDesc.presentMode = WGPUPresentMode_Fifo;
    renderer->swapchain = wgpuDeviceCreateSwapChain(renderer->device, surface, &swapDesc);
#else

#endif

    vgfxLogInfo("vgfx driver: WebGPU");

    gfxDevice_T* device = (gfxDevice_T*)VGFX_MALLOC(sizeof(gfxDevice_T));
    ASSIGN_DRIVER(webgpu);

    device->driverData = (gfxRenderer*)renderer;
    return device;
}

gfxDriver webgpu_driver = {
    VGFX_API_WEBGPU,
    webgpu_createDevice
};

#endif /* VGFX_WEBGPU_DRIVER */
