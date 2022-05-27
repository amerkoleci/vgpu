// Copyright © Amer Koleci and Contributors.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif
#else
#include "GLFW/glfw3.h"
#if defined(__linux__)
#   define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
#   define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3native.h"
#endif
#include <cstdio>
#include <cstdlib>
#include <vgpu.h>

VGPUDevice device = nullptr;
VGPUSwapChain swapChain = nullptr;
VGPUBuffer vertexBuffer = nullptr;

inline void vgpu_log(VGPULogLevel level, const char* message)
{
}

#if defined(__EMSCRIPTEN__)
namespace
{
    static EM_BOOL _app_emsc_frame(double time, void* userData)
    {
        //emscripten_log(EM_LOG_CONSOLE, "%s", "frame");
        VGFXDevice device = (VGFXDevice)userData;
        vgfxFrame(device);
        return EM_TRUE;
    }
}

void init_gfx()
#else
void init_gfx(GLFWwindow* window)
#endif
{
    VGPUDeviceDesc deviceDesc{};
    deviceDesc.label = "test device";
#ifdef _DEBUG
    deviceDesc.validationMode = VGPU_VALIDATION_MODE_ENABLED;
#endif

    //if (vgpuIsSupported(VGPU_BACKEND_TYPE_VULKAN))
    //{
    //    deviceDesc.preferredBackend = VGPU_BACKEND_TYPE_VULKAN;
    //}

    void* windowHandle = nullptr;
#if defined(__EMSCRIPTEN__)
    windowHandle = "canvas";
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
    windowHandle = glfwGetWin32Window(window);
#elif defined(GLFW_EXPOSE_NATIVE_X11)
    windowHandle = (void*)(uintptr_t)glfwGetX11Window(window);
#endif

    device = vgpuCreateDevice(&deviceDesc);

    VGPUAdapterProperties adapterProps;
    vgpuGetAdapterProperties(device, &adapterProps);

    VGPULimits limits;
    vgpuGetLimits(device, &limits);

    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);

    VGPUSwapChainDesc swapChainDesc{};
    swapChainDesc.format = VGFXTextureFormat_BGRA8UNormSrgb;
    swapChainDesc.presentMode = VGPU_PRESENT_MODE_FIFO;
    swapChain = vgpuCreateSwapChain(device, windowHandle, &swapChainDesc);

    // Create vertex buffer
    const float vertices[] = {
        /* positions            colors */
         0.0f, 0.5f, 0.5f,      1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };

    VGPUBufferDesc bufferDesc{};
    bufferDesc.label = "Vertex Buffer";
    bufferDesc.size = sizeof(vertices);
    bufferDesc.usage = VGPUBufferUsage_Vertex;
    vertexBuffer = vgpuCreateBuffer(device, &bufferDesc, vertices);
}

#if defined(__EMSCRIPTEN__)
EM_JS(void, glue_preint, (), {
    var entry = __glue_main_;
    if (entry) {
        /*
         * None of the WebGPU properties appear to survive Closure, including
         * Emscripten's own `preinitializedWebGPUDevice` (which from looking at
         *`library_html5` is probably designed to be inited in script before
         * loading the Wasm).
         */
        if (navigator["gpu"]) {
            navigator["gpu"]["requestAdapter"]().then(function(adapter) {
                adapter["requestDevice"]().then(function(device) {
                    Module["preinitializedWebGPUDevice"] = device;
                    entry();
                });
            }, function() {
                console.error("No WebGPU adapter; not starting");
            });
        }
 else {
  console.error("No support for WebGPU; not starting");
}
}
else {
 console.error("Entry point not found; unable to start");
}
    });

KEEP_IN_MODULE void _glue_main_()
{
    init_gfx();
    emscripten_request_animation_frame_loop(_app_emsc_frame, device);
}
#endif

void draw_frame()
{
    VGPUTextureFormat format = vgpuSwapChainGetFormat(device, swapChain);
    uint32_t width;
    uint32_t height;

    VGPUCommandBuffer commandBuffer = vgpuBeginCommandBuffer(device, "Frame");

    // When window minimized texture is null
    VGPUTexture swapChainTexture = vgpuAcquireSwapchainTexture(commandBuffer, swapChain, &width, &height);
    if (swapChainTexture != nullptr)
    {
        VGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.texture = swapChainTexture;
        colorAttachment.loadOp = VGPU_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VGPU_STORE_OP_STORE;
        colorAttachment.clearColor.r = 0.3f;
        colorAttachment.clearColor.g = 0.3f;
        colorAttachment.clearColor.b = 0.3f;
        colorAttachment.clearColor.a = 1.0f;

        VGPURenderPassDesc renderPass{};
        renderPass.colorAttachmentCount = 1u;
        renderPass.colorAttachments = &colorAttachment;
        vgpuBeginRenderPass(commandBuffer, &renderPass);
        vgpuEndRenderPass(commandBuffer);
    }

    vgpuSubmit(device, &commandBuffer, 1u);
    vgpuFrame(device);
}

int main()
{
#if defined(__EMSCRIPTEN__)
    glue_preint();

#else
    if (!glfwInit())
    {
        return EXIT_FAILURE;
    }

    //VGPU_SetLogCallback(vgpu_log);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
    init_gfx(window);

    while (!glfwWindowShouldClose(window))
    {
        draw_frame();
        glfwPollEvents();
    }

    vgpuWaitIdle(device);
    vgpuDestroyBuffer(device, vertexBuffer);
    vgpuDestroySwapChain(device, swapChain);
    vgpuDestroyDevice(device);
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return EXIT_SUCCESS;
}
