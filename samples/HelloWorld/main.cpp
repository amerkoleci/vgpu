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
#include <vgfx.h>

VGFXSurface surface = nullptr;
VGFXDevice device = nullptr;
VGFXSwapChain swapChain = nullptr;

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
    VGFXDeviceInfo deviceInfo{};
#ifdef _DEBUG
    deviceInfo.validationMode = VGFXValidationMode_Enabled;
#endif

    //if (vgfxIsSupported(VGFX_API_VULKAN))
    //{
    //    deviceInfo.preferredApi = VGFX_API_VULKAN;
    //}

#if defined(__EMSCRIPTEN__)
    surface = vgfxCreateSurfaceWeb("canvas");
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
    HINSTANCE hinstance = GetModuleHandle(NULL);
    HWND hwnd = glfwGetWin32Window(window);
    surface = vgfxCreateSurfaceWin32(hinstance, hwnd);
#elif defined(GLFW_EXPOSE_NATIVE_X11)
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    surface = vgfxCreateSurfaceXlib(x11_display, x11_window);
#endif

    device = vgfxCreateDevice(surface, &deviceInfo);

    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    VGFXSwapChainInfo swapChainInfo{};
    swapChainInfo.width = (uint32_t)width;
    swapChainInfo.height = (uint32_t)height;
    swapChainInfo.presentMode = VGFXPresentMode_Fifo;
    swapChain = vgfxCreateSwapChain(device, surface, &swapChainInfo);
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
    uint32_t width = vgfxSwapChainGetWidth(swapChain);
    uint32_t height = vgfxSwapChainGetHeight(swapChain);

    VGFXRenderPassColorAttachment colorAttachment = {};
    colorAttachment.texture = vgfxSwapChainGetNextTexture(swapChain);
    colorAttachment.loadOp = VGFXLoadOp_Clear;
    colorAttachment.storeOp = VGFXStoreOp_Store;
    colorAttachment.clearColor.r = 0.3f;
    colorAttachment.clearColor.g = 0.3f;
    colorAttachment.clearColor.b = 0.3f;
    colorAttachment.clearColor.a = 1.0f;

    VGFXRenderPassInfo renderPass{};
    renderPass.colorAttachmentCount = 1u;
    renderPass.colorAttachments = &colorAttachment;
    vgfxBeginRenderPass(device, &renderPass);
    vgfxEndRenderPass(device);

    vgfxFrame(device);
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

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
    init_gfx(window);

    while (!glfwWindowShouldClose(window))
    {
        draw_frame();
        glfwPollEvents();
    }

    vgfxWaitIdle(device);
    vgfxDestroySwapChain(swapChain);
    vgfxDestroyDevice(device);
    vgfxDestroySurface(surface);
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return EXIT_SUCCESS;
}
